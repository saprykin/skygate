#include "BaseCelestialBody.hpp"
#include "CelestialBodyCatalog.hpp"
#include "DistantCelestialBody.hpp"
#include "EquatorialCoordinate.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "SkyObjectSearchModel.hpp"

#include <QtTest>

#include <optional>
#include <string>
#include <vector>

namespace {

skygate::ephemeris::OwnGalaxyCelestialBody makeBody(
    std::string id,
    std::string displayName,
    const skygate::ephemeris::BaseCelestialBody::Kind type,
    const double visualMagnitude,
    const std::optional<skygate::core::EquatorialCoordinate>& fixedEquatorial = std::nullopt
)
{
    skygate::ephemeris::OwnGalaxyCelestialBody body;
    body.id = std::move(id);
    body.displayName = std::move(displayName);
    body.kind = type;
    body.visualMagnitude = visualMagnitude;
    body.fixedEquatorial = fixedEquatorial;
    return body;
}

skygate::ephemeris::DistantCelestialBody makeDeepSkyBody(
    std::string id,
    std::string displayName,
    const double visualMagnitude,
    const skygate::ephemeris::DeepSkyObjectInfo::Kind kind,
    std::vector<std::string> aliases
)
{
    skygate::ephemeris::DistantCelestialBody body;
    body.id = std::move(id);
    body.displayName = std::move(displayName);
    body.kind = skygate::ephemeris::BaseCelestialBody::Kind::DeepSkyObject;
    body.visualMagnitude = visualMagnitude;
    body.deepSkyObject = skygate::ephemeris::DeepSkyObjectInfo{.kind = kind, .aliases = std::move(aliases)};
    return body;
}

void setCatalogData(
    SkyObjectSearchModel& model,
    std::vector<skygate::ephemeris::OwnGalaxyCelestialBody> ownGalaxyBodies,
    std::vector<skygate::ephemeris::ConstellationAnchorGroup> constellationAnchorGroups
)
{
    const skygate::ephemeris::CelestialBodyCatalog catalog(std::move(ownGalaxyBodies));
    model.setCatalogData(catalog.bodies(), std::move(constellationAnchorGroups));
}

void setCatalogData(
    SkyObjectSearchModel& model,
    std::vector<skygate::ephemeris::OwnGalaxyCelestialBody> ownGalaxyBodies,
    std::vector<skygate::ephemeris::DistantCelestialBody> distantBodies,
    std::vector<skygate::ephemeris::ConstellationAnchorGroup> constellationAnchorGroups
)
{
    std::vector<skygate::ephemeris::CelestialBodyCatalog::OrderEntry> order;
    order.reserve(ownGalaxyBodies.size() + distantBodies.size());
    for (std::uint32_t index = 0U; index < ownGalaxyBodies.size(); ++index) {
        order.push_back(
            {.domain = skygate::ephemeris::CelestialBodyCatalog::BodyDomain::OwnGalaxy, .bodyIndex = index}
        );
    }
    for (std::uint32_t index = 0U; index < distantBodies.size(); ++index) {
        order.push_back({.domain = skygate::ephemeris::CelestialBodyCatalog::BodyDomain::Distant, .bodyIndex = index});
    }
    const skygate::ephemeris::CelestialBodyCatalog catalog(
        std::move(ownGalaxyBodies), std::move(distantBodies), std::move(order)
    );
    model.setCatalogData(catalog.bodies(), std::move(constellationAnchorGroups));
}

QString displayTextAt(const SkyObjectSearchModel& model, const int row)
{
    return model.index(row, 0).data(SkyObjectSearchModel::DisplayTextRole).toString();
}

QString detailTextAt(const SkyObjectSearchModel& model, const int row)
{
    return model.index(row, 0).data(SkyObjectSearchModel::DetailTextRole).toString();
}

QString targetKindAt(const SkyObjectSearchModel& model, const int row)
{
    return model.index(row, 0).data(SkyObjectSearchModel::TargetKindRole).toString();
}

QString targetIdAt(const SkyObjectSearchModel& model, const int row)
{
    return model.index(row, 0).data(SkyObjectSearchModel::TargetIdRole).toString();
}

}  // namespace

class SkyObjectSearchModelTests final : public QObject {
    Q_OBJECT

private slots:
    void blankQueryReturnsNoRows();
    void filtersPlanetStarHipAndConstellationTargets();
    void normalizesHipQueries();
    void filtersDeepSkyAliases();
    void deduplicatesDisplayNamesPreferringBodies();
    void ranksExactPrefixAndContainsMatches();
    void ranksLargerMixedCatalogWithCollisions();
};

void SkyObjectSearchModelTests::blankQueryReturnsNoRows()
{
    SkyObjectSearchModel model;
    setCatalogData(
        model,
        std::vector<skygate::ephemeris::OwnGalaxyCelestialBody>{
            makeBody("mars", "Mars", skygate::ephemeris::BaseCelestialBody::Kind::Planet, -2.0),
        },
        std::vector<skygate::ephemeris::ConstellationAnchorGroup>{}
    );

    QCOMPARE(model.rowCount(), 0);
    model.setFilterText("   ");
    QCOMPARE(model.rowCount(), 0);
}

void SkyObjectSearchModelTests::filtersPlanetStarHipAndConstellationTargets()
{
    SkyObjectSearchModel model;
    setCatalogData(
        model,
        std::vector<skygate::ephemeris::OwnGalaxyCelestialBody>{
            makeBody("mars", "Mars", skygate::ephemeris::BaseCelestialBody::Kind::Planet, -2.0),
            makeBody("sirius", "Sirius", skygate::ephemeris::BaseCelestialBody::Kind::Star, -1.46),
            makeBody("hip_77", "HIP 77", skygate::ephemeris::BaseCelestialBody::Kind::Star, 4.2),
        },
        std::vector<skygate::ephemeris::ConstellationAnchorGroup>{
            {"Orion", {"hip_77", "hip_88"}},
        }
    );

    model.setFilterText("mar");
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(displayTextAt(model, 0), QString("Mars"));
    QCOMPARE(detailTextAt(model, 0), QString("Planet"));

    model.setFilterText("sir");
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(displayTextAt(model, 0), QString("Sirius"));
    QCOMPARE(targetKindAt(model, 0), QString("body"));

    model.setFilterText("hip 77");
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(displayTextAt(model, 0), QString("HIP 77"));
    QCOMPARE(detailTextAt(model, 0), QString("Star • hip_77"));

    model.setFilterText("ori");
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(displayTextAt(model, 0), QString("Orion"));
    QCOMPARE(detailTextAt(model, 0), QString("Constellation"));
    QCOMPARE(targetKindAt(model, 0), QString("constellationLabel"));
}

void SkyObjectSearchModelTests::normalizesHipQueries()
{
    SkyObjectSearchModel model;
    setCatalogData(
        model,
        std::vector<skygate::ephemeris::OwnGalaxyCelestialBody>{
            makeBody("hip_77", "HIP 77", skygate::ephemeris::BaseCelestialBody::Kind::Star, 4.2),
        },
        std::vector<skygate::ephemeris::ConstellationAnchorGroup>{}
    );

    const QStringList queries{"hip77", "hip 77", "hip_77"};
    for (const QString& query : queries) {
        model.setFilterText(query);
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(targetIdAt(model, 0), QString("hip_77"));
    }
}

void SkyObjectSearchModelTests::filtersDeepSkyAliases()
{
    skygate::ephemeris::DistantCelestialBody m31 = makeDeepSkyBody(
        "messier_031",
        "M31",
        3.44,
        skygate::ephemeris::DeepSkyObjectInfo::Kind::Galaxy,
        {"M31", "NGC 224", "Andromeda Galaxy"}
    );
    m31.fixedEquatorial = skygate::core::EquatorialCoordinate{.rightAscensionHours = 0.7123, .declinationDeg = 41.269};

    SkyObjectSearchModel model;
    setCatalogData(
        model,
        {},
        std::vector<skygate::ephemeris::DistantCelestialBody>{m31},
        std::vector<skygate::ephemeris::ConstellationAnchorGroup>{}
    );

    model.setFilterText("andromeda");
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(displayTextAt(model, 0), QString("Andromeda Galaxy"));
    QCOMPARE(detailTextAt(model, 0), QString("Deep sky • Galaxy • messier_031"));
    QCOMPARE(targetKindAt(model, 0), QString("body"));
    QCOMPARE(targetIdAt(model, 0), QString("messier_031"));
}

void SkyObjectSearchModelTests::deduplicatesDisplayNamesPreferringBodies()
{
    SkyObjectSearchModel model;
    setCatalogData(
        model,
        std::vector<skygate::ephemeris::OwnGalaxyCelestialBody>{
            makeBody("orion_body", "Orion", skygate::ephemeris::BaseCelestialBody::Kind::Constellation, 1.0),
        },
        std::vector<skygate::ephemeris::ConstellationAnchorGroup>{
            {"Orion", {"hip_77", "hip_88"}},
        }
    );

    model.setFilterText("orion");
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(displayTextAt(model, 0), QString("Orion"));
    QCOMPARE(targetKindAt(model, 0), QString("body"));
    QCOMPARE(targetIdAt(model, 0), QString("orion_body"));
}

void SkyObjectSearchModelTests::ranksExactPrefixAndContainsMatches()
{
    SkyObjectSearchModel model;
    setCatalogData(
        model,
        std::vector<skygate::ephemeris::OwnGalaxyCelestialBody>{
            makeBody("mar", "Mar", skygate::ephemeris::BaseCelestialBody::Kind::Star, 3.0),
            makeBody("mars", "Mars", skygate::ephemeris::BaseCelestialBody::Kind::Planet, -2.0),
            makeBody("mariner", "Scout", skygate::ephemeris::BaseCelestialBody::Kind::Star, 1.0),
            makeBody("landmark", "Landmark", skygate::ephemeris::BaseCelestialBody::Kind::Star, -1.0),
        },
        std::vector<skygate::ephemeris::ConstellationAnchorGroup>{}
    );

    model.setFilterText("mar");
    QCOMPARE(model.rowCount(), 4);
    QCOMPARE(displayTextAt(model, 0), QString("Mar"));
    QCOMPARE(displayTextAt(model, 1), QString("Mars"));
    QCOMPARE(displayTextAt(model, 2), QString("Scout"));
    QCOMPARE(displayTextAt(model, 3), QString("Landmark"));
}

void SkyObjectSearchModelTests::ranksLargerMixedCatalogWithCollisions()
{
    auto m31 = makeDeepSkyBody(
        "messier_031",
        "M31",
        3.44,
        skygate::ephemeris::DeepSkyObjectInfo::Kind::Galaxy,
        {"Andromeda", "Andromeda Galaxy", "NGC 224"}
    );
    auto m57 = makeDeepSkyBody(
        "messier_057",
        "M57",
        8.8,
        skygate::ephemeris::DeepSkyObjectInfo::Kind::PlanetaryNebula,
        {"Ring Nebula", "NGC 6720"}
    );
    auto dimRingAlias = makeDeepSkyBody(
        "open_ngc_ring_duplicate",
        "Dim Ring Candidate",
        12.0,
        skygate::ephemeris::DeepSkyObjectInfo::Kind::Nebula,
        {"Ring Nebula"}
    );

    SkyObjectSearchModel model;
    setCatalogData(
        model,
        std::vector<skygate::ephemeris::OwnGalaxyCelestialBody>{
            makeBody("mars", "Mars", skygate::ephemeris::BaseCelestialBody::Kind::Planet, -2.0),
            makeBody("mercury", "Mercury", skygate::ephemeris::BaseCelestialBody::Kind::Planet, -1.0),
            makeBody("hip_24436", "Meissa", skygate::ephemeris::BaseCelestialBody::Kind::Star, 3.3),
            makeBody("hip_25930", "Mintaka", skygate::ephemeris::BaseCelestialBody::Kind::Star, 2.2),
            makeBody("andromeda_body", "Andromeda", skygate::ephemeris::BaseCelestialBody::Kind::Constellation, 99.0),
        },
        std::vector<skygate::ephemeris::DistantCelestialBody>{
            m31,
            m57,
            dimRingAlias,
        },
        std::vector<skygate::ephemeris::ConstellationAnchorGroup>{
            {"Andromeda", {"hip_24436"}},
            {"Orion", {"hip_24436", "hip_25930"}},
            {"Ring Nebula", {"hip_25930"}},
            {"No Anchors", {"hip_999999"}},
        }
    );

    model.setFilterText("andromeda");
    QVERIFY(model.rowCount() >= 2);
    QCOMPARE(displayTextAt(model, 0), QString("Andromeda"));
    QCOMPARE(targetKindAt(model, 0), QString("body"));
    QCOMPARE(targetIdAt(model, 0), QString("messier_031"));
    QCOMPARE(detailTextAt(model, 0), QString("Deep sky • Galaxy • messier_031"));
    QCOMPARE(displayTextAt(model, 1), QString("Andromeda Galaxy"));
    QCOMPARE(targetIdAt(model, 1), QString("messier_031"));

    model.setFilterText("ring nebula");
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(displayTextAt(model, 0), QString("Ring Nebula"));
    QCOMPARE(targetKindAt(model, 0), QString("body"));
    QCOMPARE(targetIdAt(model, 0), QString("messier_057"));
    QCOMPARE(detailTextAt(model, 0), QString("Deep sky • Planetary nebula • messier_057"));

    model.setFilterText("orion");
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(displayTextAt(model, 0), QString("Orion"));
    QCOMPARE(detailTextAt(model, 0), QString("Constellation"));
    QCOMPARE(targetKindAt(model, 0), QString("constellationLabel"));

    model.setFilterText("messier 031");
    QVERIFY(model.rowCount() >= 1);
    QCOMPARE(targetIdAt(model, 0), QString("messier_031"));

    model.setFilterText("no anchors");
    QCOMPARE(model.rowCount(), 0);
}

QTEST_GUILESS_MAIN(SkyObjectSearchModelTests)

#include "SkyObjectSearchModelTests.moc"
