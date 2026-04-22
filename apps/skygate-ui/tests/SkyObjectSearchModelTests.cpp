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
    void deduplicatesDisplayNamesPreferringBodies();
    void ranksExactPrefixAndContainsMatches();
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

QTEST_GUILESS_MAIN(SkyObjectSearchModelTests)

#include "SkyObjectSearchModelTests.moc"
