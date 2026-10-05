#include "CatalogSemanticFixtureAdapter.hpp"
#include "CatalogSemanticFixtureCorpus.hpp"
#include "CelestialBodyCatalog.hpp"
#include "CelestialBodyState.hpp"
#include "ConstellationTestSupport.hpp"
#include "DistantCelestialBody.hpp"
#include "EphemerisSnapshot.hpp"
#include "FakeNetworkAccessManager.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "SettingsTestFixture.hpp"
#include "SkyCatalogManager.hpp"
#include "SkyObjectInspectorFormatters.hpp"
#include "SkyObjectSearchModel.hpp"
#include "SkyOverlayLayerVisibility.hpp"
#include "SkyRenderBuilders.hpp"
#include "SkySettingsStore.hpp"
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

std::optional<std::size_t> bodyIndexById(const skygate::ephemeris::IStarCatalog* catalog, const std::string& id)
{
    if (catalog == nullptr) {
        return std::nullopt;
    }

    const auto bodies = catalog->bodies();
    for (std::size_t index = 0; index < bodies.size(); ++index) {
        if (bodies[index] != nullptr && bodies[index]->id == id) {
            return index;
        }
    }
    return std::nullopt;
}

bool labelsContainText(const SkyRenderFrame& frame, const QString& text)
{
    return std::any_of(frame.labels.begin(), frame.labels.end(), [&text](const SkyRenderLabel& label) {
        return label.text == text;
    });
}

QByteArray payloadBytes(const std::string_view payload)
{
    return QByteArray(payload.data(), static_cast<qsizetype>(payload.size()));
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

SkyRenderFrame buildFrame(
    const skygate::ephemeris::IStarCatalog& catalog,
    const std::span<const skygate::ephemeris::ConstellationLineRef> lineRefs = {},
    const std::span<const skygate::ephemeris::ConstellationAnchorGroup> anchorGroups = {}
)
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
    return builder.buildFrame(
        snapshot, *projection, lineRefs, anchorGroups, 9.0, 1000.0, 800.0, makeRenderTheme(), overlayLayers
    );
}

}  // namespace

class CatalogConsumerInterchangeabilityTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void renderInputsAgreeForEquivalentObjects();
    void searchTargetsAgreeForEquivalentObjects();
    void inspectorMetadataAgreesForEquivalentObjects();
    void ephemerisInputsAgreeForEquivalentObjects();
    void consumersAgreeOnComposedCollectionSnapshot();

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void CatalogConsumerInterchangeabilityTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("CatalogConsumerInterchangeabilityTests")));
    QVERIFY(
        skygate::ephemeris::tests::CatalogSemanticFixtureAdapter::schemaType()
        != skygate::ephemeris::CatalogSourceType::Unknown
    );
}

void CatalogConsumerInterchangeabilityTests::init()
{
    m_settings.resetSettingsWithCatalogCachePaths();
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

void CatalogConsumerInterchangeabilityTests::consumersAgreeOnComposedCollectionSnapshot()
{
    using namespace skygate::ephemeris;
    using namespace skygate::ephemeris::tests;
    using namespace skygate::ui::internal;

    const QString hygUrl = QStringLiteral("https://example.test/consumer-hyg.csv");
    const QString semanticUrl = QStringLiteral("https://example.test/consumer-semantic.tsv");
    const QString openNgcUrl = QStringLiteral("https://example.test/consumer-openngc.csv");
    const QString relatedUrl = QStringLiteral("https://example.test/consumer-lines.json");

    // The HYG encoding places the two stars where the semantic encoding does
    // not, so the surviving object is observable in every consumer instead of
    // only in the canonical identifier.
    const QByteArray hygPayload = "id,hip,proper,bf,ra,dec,mag\n"
                                  "1,32349,Sirius,Alpha Canis Majoris,1.0,2.0,-1.46\n"
                                  "2,91262,Vega,Alpha Lyrae,3.0,4.0,0.03\n";

    skygate::ui::tests::FakeNetworkAccessManager networkAccessManager;
    networkAccessManager.enqueueResponse(hygUrl, {.payload = hygPayload});
    networkAccessManager.enqueueResponse(
        semanticUrl, {.payload = payloadBytes(CatalogSemanticFixtureCorpus::semanticPayload())}
    );
    networkAccessManager.enqueueResponse(
        openNgcUrl, {.payload = payloadBytes(CatalogSemanticFixtureCorpus::openNgcDeepSkyPayload())}
    );
    networkAccessManager.enqueueResponse(
        relatedUrl,
        {.payload =
             skygate::ui::tests::stellariumConstellationIndexJsonPayload({{QStringLiteral("orion"), {32349, 91262}}})}
    );

    SkySettingsStore store;
    SkyCatalogManager manager(&store, nullptr, nullptr, &networkAccessManager);
    // The bundled deep-sky fallback stays out of this fixture so the snapshot
    // holds exactly the objects the configured sources supply.
    manager.setDeepSkyCatalogPresetIndex(1);

    SkyCatalogSourceInstance hygSource = SkyCatalogSourceInstance::createCustom(hygUrl);
    hygSource.title = QStringLiteral("HYG Fixture");
    hygSource.schemaHint = CatalogSourceType::HygCsv;
    hygSource.relatedDatasetUrls = QStringList{relatedUrl};
    manager.loadSource(hygSource, CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());

    // The bundled star source carries its own deep-sky objects, so it is
    // removed once a configured source keeps the collection non-empty.
    manager.removeSource(QStringLiteral("primary"));

    SkyCatalogSourceInstance semanticSource = SkyCatalogSourceInstance::createCustom(semanticUrl);
    semanticSource.title = QStringLiteral("Semantic Fixture");
    semanticSource.schemaHint = CatalogSemanticFixtureAdapter::schemaType();

    // The later semantic encoding wins the shared positions of both stars.
    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression(QStringLiteral(
            "Catalog composition kept the fixed coordinates of star_1 over conflicting fixed coordinates from "
            "hip_32349\\."
        ))
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression(QStringLiteral(
            "Catalog composition kept the fixed coordinates of star_2 over conflicting fixed coordinates from "
            "hip_91262\\."
        ))
    );
    manager.loadSource(semanticSource, CatalogCompositionPolicy::Merge);
    QTRY_VERIFY(!manager.downloadingCatalog());

    SkyCatalogSourceInstance openNgcSource = SkyCatalogSourceInstance::createCustom(openNgcUrl);
    openNgcSource.title = QStringLiteral("OpenNGC Fixture");
    openNgcSource.schemaHint = CatalogSourceType::OpenNgcCsv;
    manager.loadSource(openNgcSource, CatalogCompositionPolicy::DeepSkyOnly);
    QTRY_VERIFY(!manager.downloadingCatalog());
    QTRY_COMPARE(manager.constellationAnchorGroups().size(), std::size_t{1});

    const IStarCatalog* const catalog = manager.starCatalog();
    QVERIFY(catalog != nullptr);
    const auto bodies = catalog->bodies();

    // The composed survivor is the later catalog's object: the HYG spelling of
    // the same star no longer addresses a body of the active snapshot.
    const std::optional<std::size_t> starIndex = bodyIndexById(catalog, "star_1");
    const std::optional<std::size_t> m31Index = bodyIndexById(catalog, "messier_031");
    QVERIFY(starIndex.has_value());
    QVERIFY(m31Index.has_value());
    QVERIFY(bodyIndexById(catalog, "hip_32349") == std::nullopt);
    QCOMPARE(manager.sourceIds()[*starIndex], semanticSource.instanceId);
    QCOMPARE(
        manager.contributorSourceIds()[*starIndex], QStringList({semanticSource.instanceId, hygSource.instanceId})
    );
    QCOMPARE(manager.sourceIds()[*m31Index], openNgcSource.instanceId);
    QCOMPARE(
        manager.contributorSourceIds()[*m31Index], QStringList({openNgcSource.instanceId, semanticSource.instanceId})
    );

    // Search resolves the surviving object through the common domain data.
    SkyObjectSearchModel searchModel;
    searchModel.setCatalogData(bodies, manager.resolvedConstellationAnchorGroups());
    searchModel.setFilterText(QStringLiteral("sirius"));
    QCOMPARE(searchModel.rowCount(), 1);
    QCOMPARE(searchModel.index(0, 0).data(SkyObjectSearchModel::TargetIdRole).toString(), QStringLiteral("star_1"));
    QCOMPARE(searchModel.index(0, 0).data(SkyObjectSearchModel::DisplayTextRole).toString(), QStringLiteral("Sirius"));
    searchModel.setFilterText(QStringLiteral("andromeda"));
    QCOMPARE(searchModel.rowCount(), 1);
    QCOMPARE(
        searchModel.index(0, 0).data(SkyObjectSearchModel::TargetIdRole).toString(), QStringLiteral("messier_031")
    );

    // Constellation resolution maps the catalog-specific HIP references of the
    // owned dataset onto the surviving objects of the composed snapshot.
    const std::span<const ConstellationLineRef> resolvedLines = manager.resolvedConstellationLineRefs();
    const std::span<const ConstellationAnchorGroup> resolvedAnchors = manager.resolvedConstellationAnchorGroups();
    QCOMPARE(resolvedLines.size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(resolvedLines.front().first), QStringLiteral("star_1"));
    QCOMPARE(QString::fromStdString(resolvedLines.front().second), QStringLiteral("star_2"));
    QCOMPARE(resolvedAnchors.size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(resolvedAnchors.front().first), QStringLiteral("Orion"));
    QCOMPARE(resolvedAnchors.front().second, (std::vector<std::string>{"star_1", "star_2"}));

    // The rendering-independent scene data built from that snapshot and the
    // resolved related data references the same surviving objects and labels.
    const SkyRenderFrame frame = buildFrame(*catalog, resolvedLines, resolvedAnchors);
    QCOMPARE(frame.lines.size(), std::size_t{1});
    const auto starPoint =
        std::find_if(frame.points.begin(), frame.points.end(), [&starIndex](const SkyRenderPoint& point) {
            return point.bodyIndex == *starIndex;
        });
    QVERIFY(starPoint != frame.points.end());
    const auto m31Glyph =
        std::find_if(frame.glyphs.begin(), frame.glyphs.end(), [&m31Index](const SkyRenderGlyph& glyph) {
            return glyph.bodyIndex == *m31Index;
        });
    QVERIFY(m31Glyph != frame.glyphs.end());
    QVERIFY(labelsContainText(frame, QStringLiteral("M31")));

    // A consumer that kept using the raw catalog-specific spelling would draw
    // no constellation segment at all: the spelling addresses no body of the
    // composed snapshot.
    const std::vector<ConstellationLineRef> rawLines{{"hip_32349", "hip_91262"}};
    const SkyRenderFrame unresolvedFrame = buildFrame(*catalog, rawLines);
    QVERIFY(unresolvedFrame.lines.empty());

    // The inspector's source field reports the same surviving provenance.
    QCOMPARE(
        sourceLabelForBodyIndex(
            manager.sourceIds(),
            &manager.contributorSourceIds(),
            manager.sourceTitles(),
            static_cast<std::uint32_t>(*starIndex)
        ),
        QStringLiteral("Semantic Fixture + HYG Fixture")
    );
    QCOMPARE(
        sourceLabelForBodyIndex(
            manager.sourceIds(),
            &manager.contributorSourceIds(),
            manager.sourceTitles(),
            static_cast<std::uint32_t>(*m31Index)
        ),
        QStringLiteral("OpenNGC Fixture + Semantic Fixture")
    );

    // The ephemeris input of the same surviving object uses the winner's own
    // coordinates instead of the absorbed catalog's.
    const SimpleBodyStateCalculator calculator;
    const auto state = calculator.computeEquatorial(*bodies[*starIndex], skygate::core::UtcTimePoint{});
    QVERIFY(state.has_value());
    QVERIFY(nearEqual(state->rightAscensionHours, 6.7525, 1e-3));
    QVERIFY(nearEqual(state->declinationDeg, -16.7161, 1e-2));
}

QTEST_GUILESS_MAIN(CatalogConsumerInterchangeabilityTests)

#include "CatalogConsumerInterchangeabilityTests.moc"
