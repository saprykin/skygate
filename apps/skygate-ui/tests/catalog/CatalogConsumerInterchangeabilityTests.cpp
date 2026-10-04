#include "CatalogSemanticFixtureAdapter.hpp"
#include "CatalogSemanticFixtureCorpus.hpp"
#include "CelestialBodyCatalog.hpp"
#include "CelestialBodyState.hpp"
#include "DistantCelestialBody.hpp"
#include "EphemerisSnapshot.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "SkyObjectInspectorFormatters.hpp"
#include "SkyObjectSearchModel.hpp"
#include "SkyOverlayLayerVisibility.hpp"
#include "SkyRenderBuilders.hpp"
#include "SkyTheme.hpp"
#include "catalog/CatalogFactory.hpp"
#include "catalog/CatalogIdentifier.hpp"
#include "catalog/CatalogPayloadParser.hpp"
#include "catalog/IStarCatalog.hpp"
#include "math/ViewportMath.hpp"
#include "reference/SimpleBodyStateCalculator.hpp"

#include <QColor>
#include <QtTest/QtTest>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

using skygate::ephemeris::BaseCelestialBody;
using skygate::ephemeris::CelestialBodyCatalog;
using skygate::ephemeris::CelestialBodyState;
using skygate::ephemeris::DistantCelestialBody;
using skygate::ephemeris::EphemerisSnapshot;
using skygate::ephemeris::OwnGalaxyCelestialBody;

bool nearEqual(const double lhs, const double rhs, const double tolerance)
{
    return std::abs(lhs - rhs) <= tolerance;
}

bool hasIdentifier(const BaseCelestialBody& body, const std::string_view namespaceName, const std::string_view value)
{
    return std::any_of(
        body.identity.externalIdentifiers.begin(),
        body.identity.externalIdentifiers.end(),
        [&](const skygate::ephemeris::CatalogIdentifier& identifier) {
            return identifier.namespaceName == namespaceName && identifier.value == value;
        }
    );
}

const BaseCelestialBody* findByExternalIdentifier(
    const std::span<const BaseCelestialBody* const> bodies,
    const std::string_view namespaceName,
    const std::string_view value
)
{
    const auto it = std::find_if(bodies.begin(), bodies.end(), [&](const BaseCelestialBody* body) {
        return body != nullptr && hasIdentifier(*body, namespaceName, value);
    });
    return it == bodies.end() ? nullptr : *it;
}

std::unique_ptr<skygate::ephemeris::IStarCatalog>
filterBodies(const skygate::ephemeris::IStarCatalog& source, const BaseCelestialBody::Kind kind)
{
    std::vector<OwnGalaxyCelestialBody> ownGalaxyBodies;
    std::vector<DistantCelestialBody> distantBodies;
    std::vector<CelestialBodyCatalog::OrderEntry> order;
    for (const BaseCelestialBody* body : source.bodies()) {
        if (body == nullptr || body->kind != kind) {
            continue;
        }
        if (kind == BaseCelestialBody::Kind::DeepSkyObject) {
            order.push_back(
                CelestialBodyCatalog::OrderEntry{
                    .domain = CelestialBodyCatalog::BodyDomain::Distant, .bodyIndex = distantBodies.size()
                }
            );
            distantBodies.push_back(CelestialBodyCatalog::copyDistantBody(*body));
        } else {
            order.push_back(
                CelestialBodyCatalog::OrderEntry{
                    .domain = CelestialBodyCatalog::BodyDomain::OwnGalaxy, .bodyIndex = ownGalaxyBodies.size()
                }
            );
            ownGalaxyBodies.push_back(CelestialBodyCatalog::copyOwnGalaxyBody(*body));
        }
    }
    return skygate::ephemeris::CatalogFactory::createStarCatalogFromBodies(
        std::move(ownGalaxyBodies), std::move(distantBodies), std::move(order)
    );
}

skygate::ui::internal::SkyThemeRenderPalette makeRenderTheme()
{
    skygate::ui::internal::SkyThemeRenderPalette theme;
    theme.bodyStar = QColor("#ffffff");
    theme.bodyDeepSkyObject = QColor("#00ffff");
    theme.bodyPlanet = QColor("#ffcc66");
    theme.bodyConstellation = QColor("#8888ff");
    theme.labelDeepSkyObject = QColor("#00ffff");
    theme.labelPlanet = QColor("#ffcc66");
    theme.labelConstellation = QColor("#8888ff");
    theme.labelDefault = QColor("#ffffff");
    theme.constellationLine = QColor("#4455ff");
    return theme;
}

SkyRenderFrame buildFrame(const skygate::ephemeris::IStarCatalog& catalog)
{
    EphemerisSnapshot snapshot;
    snapshot.catalogBodies = std::make_shared<const CelestialBodyCatalog>(catalog.catalog());
    for (std::uint32_t index = 0; index < catalog.catalog().size(); ++index) {
        snapshot.states.push_back(
            CelestialBodyState{
                .bodyIndex = index,
                .horizontal = {.altitudeDeg = 45.0, .azimuthDeg = 180.0},
            }
        );
    }

    const auto projection = skygate::core::PreparedProjection::create(
        skygate::core::ProjectionType::Stereographic,
        skygate::core::ViewportMath::buildProjectionParams(1000.0, 800.0, 45.0, 180.0, 40.0)
    );

    SkyRenderFrameBuilder builder;
    SkyOverlayLayerVisibility overlayLayers;
    return builder.buildFrame(snapshot, *projection, {}, {}, 9.0, 1000.0, 800.0, makeRenderTheme(), overlayLayers);
}

}  // namespace

class CatalogConsumerInterchangeabilityTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void renderInputsAgreeForEquivalentObjects();
    void searchTargetsAgreeForEquivalentObjects();
    void inspectorMetadataAgreesForEquivalentObjects();
    void ephemerisInputsAgreeForEquivalentObjects();
};

void CatalogConsumerInterchangeabilityTests::initTestCase()
{
    QVERIFY(
        skygate::ephemeris::tests::CatalogSemanticFixtureAdapter::schemaType()
        != skygate::ephemeris::CatalogSourceType::Unknown
    );
}

void CatalogConsumerInterchangeabilityTests::renderInputsAgreeForEquivalentObjects()
{
    using namespace skygate::ephemeris;
    using namespace skygate::ephemeris::tests;

    const CatalogPayloadParser parser;
    const auto hyg = parser.parseResult(CatalogSemanticFixtureCorpus::hygStarPayload());
    const auto openNgc = parser.parseResult(CatalogSemanticFixtureCorpus::openNgcDeepSkyPayload());
    const auto semantic = parser.parseResult(CatalogSemanticFixtureCorpus::semanticPayload());
    QVERIFY2(hyg.isSuccess(), hyg.errorDetail.c_str());
    QVERIFY2(openNgc.isSuccess(), openNgc.errorDetail.c_str());
    QVERIFY2(semantic.isSuccess(), semantic.errorDetail.c_str());

    auto semanticStars = filterBodies(*semantic.catalog, BaseCelestialBody::Kind::Star);
    auto semanticDsos = filterBodies(*semantic.catalog, BaseCelestialBody::Kind::DeepSkyObject);
    QVERIFY(semanticStars != nullptr);
    QVERIFY(semanticDsos != nullptr);

    const SkyRenderFrame hygFrame = buildFrame(*hyg.catalog);
    const SkyRenderFrame semanticStarFrame = buildFrame(*semanticStars);
    QCOMPARE(hygFrame.points.size(), std::size_t{2});
    QCOMPARE(semanticStarFrame.points.size(), std::size_t{2});
    for (std::size_t index = 0; index < hygFrame.points.size(); ++index) {
        const SkyRenderPoint& lhs = hygFrame.points[index];
        const SkyRenderPoint& rhs = semanticStarFrame.points[index];
        QCOMPARE(lhs.bodyIndex, rhs.bodyIndex);
        QCOMPARE(lhs.sizePx, rhs.sizePx);
        QCOMPARE(lhs.color.name(), rhs.color.name());
        QVERIFY(nearEqual(lhs.x, rhs.x, 1e-6));
        QVERIFY(nearEqual(lhs.y, rhs.y, 1e-6));
    }

    const SkyRenderFrame openNgcFrame = buildFrame(*openNgc.catalog);
    const SkyRenderFrame semanticDsoFrame = buildFrame(*semanticDsos);
    QCOMPARE(openNgcFrame.glyphs.size(), std::size_t{2});
    QCOMPARE(semanticDsoFrame.glyphs.size(), std::size_t{2});
    for (std::size_t index = 0; index < openNgcFrame.glyphs.size(); ++index) {
        const SkyRenderGlyph& lhs = openNgcFrame.glyphs[index];
        const SkyRenderGlyph& rhs = semanticDsoFrame.glyphs[index];
        QCOMPARE(lhs.bodyIndex, rhs.bodyIndex);
        QCOMPARE(static_cast<int>(lhs.kind), static_cast<int>(rhs.kind));
        QCOMPARE(lhs.radiusXPx, rhs.radiusXPx);
        QCOMPARE(lhs.radiusYPx, rhs.radiusYPx);
        QCOMPARE(lhs.rotationDeg, rhs.rotationDeg);
        QCOMPARE(lhs.color.name(), rhs.color.name());
        QVERIFY(nearEqual(lhs.x, rhs.x, 1e-6));
        QVERIFY(nearEqual(lhs.y, rhs.y, 1e-6));
    }
}

void CatalogConsumerInterchangeabilityTests::searchTargetsAgreeForEquivalentObjects()
{
    using namespace skygate::ephemeris;
    using namespace skygate::ephemeris::tests;

    const CatalogPayloadParser parser;
    const auto hyg = parser.parseResult(CatalogSemanticFixtureCorpus::hygStarPayload());
    const auto openNgc = parser.parseResult(CatalogSemanticFixtureCorpus::openNgcDeepSkyPayload());
    const auto semantic = parser.parseResult(CatalogSemanticFixtureCorpus::semanticPayload());
    QVERIFY2(hyg.isSuccess(), hyg.errorDetail.c_str());
    QVERIFY2(openNgc.isSuccess(), openNgc.errorDetail.c_str());
    QVERIFY2(semantic.isSuccess(), semantic.errorDetail.c_str());

    const std::vector<ConstellationAnchorGroup> noAnchors;

    SkyObjectSearchModel hygModel;
    hygModel.setCatalogData(hyg.catalog->bodies(), noAnchors);
    SkyObjectSearchModel semanticModel;
    semanticModel.setCatalogData(semantic.catalog->bodies(), noAnchors);

    hygModel.setFilterText("sirius");
    semanticModel.setFilterText("sirius");
    QCOMPARE(hygModel.rowCount(), 1);
    QCOMPARE(semanticModel.rowCount(), 1);
    QCOMPARE(
        hygModel.index(0, 0).data(SkyObjectSearchModel::DisplayTextRole).toString(),
        semanticModel.index(0, 0).data(SkyObjectSearchModel::DisplayTextRole).toString()
    );
    QCOMPARE(
        hygModel.index(0, 0).data(SkyObjectSearchModel::TargetKindRole).toString(),
        semanticModel.index(0, 0).data(SkyObjectSearchModel::TargetKindRole).toString()
    );
    QCOMPARE(hygModel.index(0, 0).data(SkyObjectSearchModel::TargetIdRole).toString(), QStringLiteral("hip_32349"));
    QCOMPARE(semanticModel.index(0, 0).data(SkyObjectSearchModel::TargetIdRole).toString(), QStringLiteral("star_1"));

    // Deep-sky aliases resolve to one canonical object per source.
    SkyObjectSearchModel openNgcModel;
    openNgcModel.setCatalogData(openNgc.catalog->bodies(), noAnchors);
    openNgcModel.setFilterText("andromeda");
    semanticModel.setFilterText("andromeda");
    QCOMPARE(openNgcModel.rowCount(), 1);
    QCOMPARE(semanticModel.rowCount(), 1);
    QCOMPARE(
        openNgcModel.index(0, 0).data(SkyObjectSearchModel::DisplayTextRole).toString(),
        semanticModel.index(0, 0).data(SkyObjectSearchModel::DisplayTextRole).toString()
    );
    QCOMPARE(
        openNgcModel.index(0, 0).data(SkyObjectSearchModel::TargetIdRole).toString(), QStringLiteral("messier_031")
    );
    QCOMPARE(semanticModel.index(0, 0).data(SkyObjectSearchModel::TargetIdRole).toString(), QStringLiteral("dso_3"));
}

void CatalogConsumerInterchangeabilityTests::inspectorMetadataAgreesForEquivalentObjects()
{
    using namespace skygate::ephemeris;
    using namespace skygate::ephemeris::tests;
    using namespace skygate::ui::internal;

    const CatalogPayloadParser parser;
    const auto hyg = parser.parseResult(CatalogSemanticFixtureCorpus::hygStarPayload());
    const auto openNgc = parser.parseResult(CatalogSemanticFixtureCorpus::openNgcDeepSkyPayload());
    const auto semantic = parser.parseResult(CatalogSemanticFixtureCorpus::semanticPayload());
    QVERIFY2(hyg.isSuccess(), hyg.errorDetail.c_str());
    QVERIFY2(openNgc.isSuccess(), openNgc.errorDetail.c_str());
    QVERIFY2(semantic.isSuccess(), semantic.errorDetail.c_str());

    const BaseCelestialBody* hygSirius = findByExternalIdentifier(hyg.catalog->bodies(), "hip", "32349");
    const BaseCelestialBody* semanticSirius = findByExternalIdentifier(semantic.catalog->bodies(), "hip", "32349");
    QVERIFY(hygSirius != nullptr);
    QVERIFY(semanticSirius != nullptr);
    QCOMPARE(celestialBodyTypeText(*hygSirius), celestialBodyTypeText(*semanticSirius));
    QCOMPARE(formatMagnitude(hygSirius->visualMagnitude), formatMagnitude(semanticSirius->visualMagnitude));
    QCOMPARE(
        formatEquatorialCoordinate(*hygSirius->fixedEquatorialValue()),
        formatEquatorialCoordinate(*semanticSirius->fixedEquatorialValue())
    );

    const BaseCelestialBody* openNgcAndromeda = findByExternalIdentifier(openNgc.catalog->bodies(), "messier", "031");
    const BaseCelestialBody* semanticAndromeda = findByExternalIdentifier(semantic.catalog->bodies(), "messier", "031");
    QVERIFY(openNgcAndromeda != nullptr);
    QVERIFY(semanticAndromeda != nullptr);
    QCOMPARE(celestialBodyTypeText(*openNgcAndromeda), celestialBodyTypeText(*semanticAndromeda));
    QCOMPARE(formatMagnitude(openNgcAndromeda->visualMagnitude), formatMagnitude(semanticAndromeda->visualMagnitude));
    QCOMPARE(
        formatEquatorialCoordinate(*openNgcAndromeda->fixedEquatorialValue()),
        formatEquatorialCoordinate(*semanticAndromeda->fixedEquatorialValue())
    );
    QCOMPARE(
        angularSizeText(*openNgcAndromeda->deepSkyObjectValue()),
        angularSizeText(*semanticAndromeda->deepSkyObjectValue())
    );
}

void CatalogConsumerInterchangeabilityTests::ephemerisInputsAgreeForEquivalentObjects()
{
    using namespace skygate::ephemeris;
    using namespace skygate::ephemeris::tests;

    const CatalogPayloadParser parser;
    const auto hyg = parser.parseResult(CatalogSemanticFixtureCorpus::hygStarPayload());
    const auto openNgc = parser.parseResult(CatalogSemanticFixtureCorpus::openNgcDeepSkyPayload());
    const auto semantic = parser.parseResult(CatalogSemanticFixtureCorpus::semanticPayload());
    QVERIFY2(hyg.isSuccess(), hyg.errorDetail.c_str());
    QVERIFY2(openNgc.isSuccess(), openNgc.errorDetail.c_str());
    QVERIFY2(semantic.isSuccess(), semantic.errorDetail.c_str());

    const SimpleBodyStateCalculator calculator;
    const skygate::core::UtcTimePoint utcTime{};

    const auto assertEquivalentState = [&calculator,
                                        &utcTime](const BaseCelestialBody* lhs, const BaseCelestialBody* rhs) {
        QVERIFY(lhs != nullptr);
        QVERIFY(rhs != nullptr);
        const auto lhsState = calculator.computeEquatorial(*lhs, utcTime);
        const auto rhsState = calculator.computeEquatorial(*rhs, utcTime);
        QVERIFY(lhsState.has_value());
        QVERIFY(rhsState.has_value());
        QVERIFY(nearEqual(lhsState->rightAscensionHours, rhsState->rightAscensionHours, 1e-3));
        QVERIFY(nearEqual(lhsState->declinationDeg, rhsState->declinationDeg, 1e-3));
    };

    assertEquivalentState(
        findByExternalIdentifier(hyg.catalog->bodies(), "hip", "32349"),
        findByExternalIdentifier(semantic.catalog->bodies(), "hip", "32349")
    );
    assertEquivalentState(
        findByExternalIdentifier(hyg.catalog->bodies(), "hip", "91262"),
        findByExternalIdentifier(semantic.catalog->bodies(), "hip", "91262")
    );
    assertEquivalentState(
        findByExternalIdentifier(openNgc.catalog->bodies(), "messier", "031"),
        findByExternalIdentifier(semantic.catalog->bodies(), "messier", "031")
    );
    assertEquivalentState(
        findByExternalIdentifier(openNgc.catalog->bodies(), "messier", "057"),
        findByExternalIdentifier(semantic.catalog->bodies(), "messier", "057")
    );
}

QTEST_GUILESS_MAIN(CatalogConsumerInterchangeabilityTests)

#include "CatalogConsumerInterchangeabilityTests.moc"
