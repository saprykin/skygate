#include <QtTest>

#include "SkyObjectSearchModel.hpp"

#include "skygate/core/Types.hpp"
#include "skygate/ephemeris/Types.hpp"

#include <optional>
#include <string>
#include <vector>

namespace {

skygate::ephemeris::CelestialBody makeBody(
    std::string id,
    std::string displayName,
    const skygate::ephemeris::CelestialBodyType type,
    const double visualMagnitude,
    const std::optional<skygate::core::EquatorialCoordinate>& fixedEquatorial = std::nullopt
)
{
    skygate::ephemeris::CelestialBody body;
    body.id = std::move(id);
    body.displayName = std::move(displayName);
    body.type = type;
    body.visualMagnitude = visualMagnitude;
    body.fixedEquatorial = fixedEquatorial;
    return body;
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
    model.setCatalogData(
        std::vector<skygate::ephemeris::CelestialBody> {
            makeBody("mars", "Mars", skygate::ephemeris::CelestialBodyType::Planet, -2.0),
        },
        std::vector<skygate::ephemeris::ConstellationLabelRef> {}
    );

    QCOMPARE(model.rowCount(), 0);
    model.setFilterText("   ");
    QCOMPARE(model.rowCount(), 0);
}

void SkyObjectSearchModelTests::filtersPlanetStarHipAndConstellationTargets()
{
    SkyObjectSearchModel model;
    model.setCatalogData(
        std::vector<skygate::ephemeris::CelestialBody> {
            makeBody("mars", "Mars", skygate::ephemeris::CelestialBodyType::Planet, -2.0),
            makeBody("sirius", "Sirius", skygate::ephemeris::CelestialBodyType::Star, -1.46),
            makeBody("hip_77", "HIP 77", skygate::ephemeris::CelestialBodyType::Star, 4.2),
        },
        std::vector<skygate::ephemeris::ConstellationLabelRef> {
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
    model.setCatalogData(
        std::vector<skygate::ephemeris::CelestialBody> {
            makeBody("hip_77", "HIP 77", skygate::ephemeris::CelestialBodyType::Star, 4.2),
        },
        std::vector<skygate::ephemeris::ConstellationLabelRef> {}
    );

    const QStringList queries {"hip77", "hip 77", "hip_77"};
    for (const QString& query : queries) {
        model.setFilterText(query);
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(targetIdAt(model, 0), QString("hip_77"));
    }
}

void SkyObjectSearchModelTests::filtersDeepSkyAliases()
{
    skygate::ephemeris::CelestialBody m31 = makeBody(
        "messier_031",
        "M31",
        skygate::ephemeris::CelestialBodyType::DeepSkyObject,
        3.44,
        skygate::core::EquatorialCoordinate {
            .rightAscensionHours = 0.7123,
            .declinationDeg = 41.269
        }
    );
    m31.deepSkyObject = skygate::ephemeris::DeepSkyObjectInfo {
        .kind = skygate::ephemeris::DeepSkyObjectKind::Galaxy,
        .aliases = {"M31", "NGC 224", "Andromeda Galaxy"},
    };

    SkyObjectSearchModel model;
    model.setCatalogData(
        std::vector<skygate::ephemeris::CelestialBody> {m31},
        std::vector<skygate::ephemeris::ConstellationLabelRef> {}
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
    model.setCatalogData(
        std::vector<skygate::ephemeris::CelestialBody> {
            makeBody("orion_body", "Orion", skygate::ephemeris::CelestialBodyType::Constellation, 1.0),
        },
        std::vector<skygate::ephemeris::ConstellationLabelRef> {
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
    model.setCatalogData(
        std::vector<skygate::ephemeris::CelestialBody> {
            makeBody("mar", "Mar", skygate::ephemeris::CelestialBodyType::Star, 3.0),
            makeBody("mars", "Mars", skygate::ephemeris::CelestialBodyType::Planet, -2.0),
            makeBody("mariner", "Scout", skygate::ephemeris::CelestialBodyType::Star, 1.0),
            makeBody("landmark", "Landmark", skygate::ephemeris::CelestialBodyType::Star, -1.0),
        },
        std::vector<skygate::ephemeris::ConstellationLabelRef> {}
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
    auto m31 = makeBody(
        "messier_031",
        "M31",
        skygate::ephemeris::CelestialBodyType::DeepSkyObject,
        3.44
    );
    m31.deepSkyObject = skygate::ephemeris::DeepSkyObjectInfo {
        .kind = skygate::ephemeris::DeepSkyObjectKind::Galaxy,
        .aliases = {"Andromeda", "Andromeda Galaxy", "NGC 224"},
    };
    auto m57 = makeBody(
        "messier_057",
        "M57",
        skygate::ephemeris::CelestialBodyType::DeepSkyObject,
        8.8
    );
    m57.deepSkyObject = skygate::ephemeris::DeepSkyObjectInfo {
        .kind = skygate::ephemeris::DeepSkyObjectKind::PlanetaryNebula,
        .aliases = {"Ring Nebula", "NGC 6720"},
    };
    auto dimRingAlias = makeBody(
        "open_ngc_ring_duplicate",
        "Dim Ring Candidate",
        skygate::ephemeris::CelestialBodyType::DeepSkyObject,
        12.0
    );
    dimRingAlias.deepSkyObject = skygate::ephemeris::DeepSkyObjectInfo {
        .kind = skygate::ephemeris::DeepSkyObjectKind::Nebula,
        .aliases = {"Ring Nebula"},
    };

    SkyObjectSearchModel model;
    model.setCatalogData(
        std::vector<skygate::ephemeris::CelestialBody> {
            makeBody("mars", "Mars", skygate::ephemeris::CelestialBodyType::Planet, -2.0),
            makeBody("mercury", "Mercury", skygate::ephemeris::CelestialBodyType::Planet, -1.0),
            makeBody("hip_24436", "Meissa", skygate::ephemeris::CelestialBodyType::Star, 3.3),
            makeBody("hip_25930", "Mintaka", skygate::ephemeris::CelestialBodyType::Star, 2.2),
            makeBody("andromeda_body", "Andromeda", skygate::ephemeris::CelestialBodyType::Constellation, 99.0),
            m31,
            m57,
            dimRingAlias,
        },
        std::vector<skygate::ephemeris::ConstellationLabelRef> {
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
    QCOMPARE(
        detailTextAt(model, 0),
        QString("Deep sky • Planetary nebula • messier_057")
    );

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
