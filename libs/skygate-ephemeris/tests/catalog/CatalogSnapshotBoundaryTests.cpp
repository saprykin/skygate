#include "CelestialBodyCatalog.hpp"
#include "DistantCelestialBody.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "catalog/IStarCatalog.hpp"
#include "factory/EphemerisEngineFactory.hpp"

#include <QtTest/QtTest>

#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace {

using skygate::core::EquatorialCoordinate;
using skygate::core::ObservationContext;
using skygate::core::UtcTimePoint;
using skygate::ephemeris::BaseCelestialBody;
using skygate::ephemeris::CelestialBodyCatalog;
using skygate::ephemeris::DeepSkyObjectInfo;
using skygate::ephemeris::DistantCelestialBody;
using skygate::ephemeris::OwnGalaxyCelestialBody;

// Alternative in-memory provider that does not use CelestialBodyCatalog as its
// primary storage. Consumers must be able to read it through the common body
// view without naming this concrete type.
class AlternativeInMemoryStarCatalog final : public skygate::ephemeris::IStarCatalog {
public:
    AlternativeInMemoryStarCatalog(
        std::vector<OwnGalaxyCelestialBody> ownGalaxyBodies, std::vector<DistantCelestialBody> distantBodies
    )
        : m_ownGalaxyBodies(std::move(ownGalaxyBodies)), m_distantBodies(std::move(distantBodies))
    {
        m_orderedBodies.reserve(m_ownGalaxyBodies.size() + m_distantBodies.size());
        for (const OwnGalaxyCelestialBody& body : m_ownGalaxyBodies) {
            m_orderedBodies.push_back(&body);
        }
        for (const DistantCelestialBody& body : m_distantBodies) {
            m_orderedBodies.push_back(&body);
        }

        m_catalog = CelestialBodyCatalog(std::span<const BaseCelestialBody* const>{m_orderedBodies});
    }

    [[nodiscard]] const CelestialBodyCatalog& catalog() const noexcept override
    {
        return m_catalog;
    }

    [[nodiscard]] std::span<const BaseCelestialBody* const> bodies() const override
    {
        return m_orderedBodies;
    }

private:
    std::vector<OwnGalaxyCelestialBody> m_ownGalaxyBodies;
    std::vector<DistantCelestialBody> m_distantBodies;
    std::vector<const BaseCelestialBody*> m_orderedBodies;
    CelestialBodyCatalog m_catalog;
};

[[nodiscard]] OwnGalaxyCelestialBody makeSnapshotBoundaryStar()
{
    OwnGalaxyCelestialBody body;
    body.id = "alt_star";
    body.displayName = "Alternative Star";
    body.kind = BaseCelestialBody::Kind::Star;
    body.fixedEquatorial = EquatorialCoordinate{
        .rightAscensionHours = 5.25,
        .declinationDeg = -12.75,
    };
    return body;
}

[[nodiscard]] DistantCelestialBody makeSnapshotBoundaryDeepSkyObject()
{
    DistantCelestialBody body;
    body.id = "alt_dso";
    body.displayName = "Alternative DSO";
    body.kind = BaseCelestialBody::Kind::DeepSkyObject;
    body.fixedEquatorial = EquatorialCoordinate{
        .rightAscensionHours = 0.7,
        .declinationDeg = 41.3,
    };
    body.deepSkyObject = DeepSkyObjectInfo{
        .kind = DeepSkyObjectInfo::Kind::Galaxy,
        .aliases = {"ALT DSO"},
    };
    return body;
}

[[nodiscard]] ObservationContext makeObservationContext()
{
    ObservationContext context;
    context.utcTime = UtcTimePoint(std::chrono::seconds(1'704'067'200));
    context.observer = {
        .latitudeDeg = 37.7749,
        .longitudeDeg = -122.4194,
        .elevationMeters = 10.0,
    };
    return context;
}

}  // namespace

class CatalogSnapshotBoundaryTests final : public QObject {
    Q_OBJECT

private slots:
    void factoryConsumesAlternativeProviderThroughCommonBodyView();
    void engineSnapshotRemainsValidAfterProviderOwnerChange();
    void catalogSnapshotCopiesBodiesIndependentlyOfProvider();
};

void CatalogSnapshotBoundaryTests::factoryConsumesAlternativeProviderThroughCommonBodyView()
{
    const ObservationContext context = makeObservationContext();
    AlternativeInMemoryStarCatalog provider(
        std::vector<OwnGalaxyCelestialBody>{makeSnapshotBoundaryStar()},
        std::vector<DistantCelestialBody>{makeSnapshotBoundaryDeepSkyObject()}
    );

    const skygate::ephemeris::EphemerisEngineFactoryResult result =
        skygate::ephemeris::EphemerisEngineFactory::create(provider);

    QVERIFY(result.isSuccess());
    QVERIFY(result.engine != nullptr);

    const auto starState = result.engine->computeBodyState(context, "alt_star");
    QVERIFY(starState.has_value());
    QCOMPARE(starState->bodyIndex, 0U);
    QCOMPARE(starState->equatorial.rightAscensionHours, 5.25);
    QCOMPARE(starState->equatorial.declinationDeg, -12.75);

    const auto deepSkyState = result.engine->computeBodyState(context, std::size_t{1});
    QVERIFY(deepSkyState.has_value());
    QCOMPARE(deepSkyState->bodyIndex, 1U);
    QCOMPARE(deepSkyState->equatorial.rightAscensionHours, 0.7);
    QCOMPARE(deepSkyState->equatorial.declinationDeg, 41.3);
}

void CatalogSnapshotBoundaryTests::engineSnapshotRemainsValidAfterProviderOwnerChange()
{
    const ObservationContext context = makeObservationContext();
    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> engine;
    {
        AlternativeInMemoryStarCatalog provider(
            std::vector<OwnGalaxyCelestialBody>{makeSnapshotBoundaryStar()},
            std::vector<DistantCelestialBody>{makeSnapshotBoundaryDeepSkyObject()}
        );
        auto result = skygate::ephemeris::EphemerisEngineFactory::create(provider);
        QVERIFY(result.isSuccess());
        engine = std::move(result.engine);
    }

    // The provider owner is gone; the engine's materialized snapshot must keep
    // its body indexes and IDs resolvable.
    QVERIFY(engine != nullptr);

    const auto byIndex = engine->computeBodyState(context, std::size_t{0});
    QVERIFY(byIndex.has_value());
    QCOMPARE(byIndex->bodyIndex, 0U);
    QCOMPARE(byIndex->equatorial.rightAscensionHours, 5.25);
    QCOMPARE(byIndex->equatorial.declinationDeg, -12.75);

    const auto byId = engine->computeBodyState(context, "alt_dso");
    QVERIFY(byId.has_value());
    QCOMPARE(byId->bodyIndex, 1U);
    QCOMPARE(byId->equatorial.rightAscensionHours, 0.7);
    QCOMPARE(byId->equatorial.declinationDeg, 41.3);
}

void CatalogSnapshotBoundaryTests::catalogSnapshotCopiesBodiesIndependentlyOfProvider()
{
    std::optional<CelestialBodyCatalog> snapshot;
    {
        AlternativeInMemoryStarCatalog provider(
            std::vector<OwnGalaxyCelestialBody>{makeSnapshotBoundaryStar()},
            std::vector<DistantCelestialBody>{makeSnapshotBoundaryDeepSkyObject()}
        );
        snapshot.emplace(provider.bodies());
    }

    QVERIFY(snapshot.has_value());
    QCOMPARE(snapshot->size(), std::size_t{2});
    QCOMPARE(QString::fromStdString(snapshot->bodyAt(0).id), QStringLiteral("alt_star"));
    QCOMPARE(QString::fromStdString(snapshot->bodyAt(1).id), QStringLiteral("alt_dso"));
    QCOMPARE(snapshot->ownGalaxyBodies().size(), std::size_t{1});
    QCOMPARE(snapshot->distantBodies().size(), std::size_t{1});
}

QTEST_APPLESS_MAIN(CatalogSnapshotBoundaryTests)

#include "CatalogSnapshotBoundaryTests.moc"
