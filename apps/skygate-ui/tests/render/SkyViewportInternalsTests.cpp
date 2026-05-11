#include "SkyViewportGeometry.hpp"
#include "SkyViewportSceneGraph.hpp"

#include <QtTest/QtTest>

#include "skygate/core/math/ViewportMath.hpp"

#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>

#include <algorithm>
#include <optional>
#include <vector>

namespace {

std::optional<skygate::core::PreparedProjection> makeProjection()
{
    return skygate::core::PreparedProjection::create(
        skygate::core::ProjectionType::Stereographic,
        skygate::core::ViewportMath::buildProjectionParams(800.0, 600.0, 45.0, 180.0, 80.0)
    );
}

SkyOverlayLayerVisibility hiddenOverlays()
{
    SkyOverlayLayerVisibility overlays;
    overlays.horizon = false;
    overlays.altAzGrid = false;
    overlays.ecliptic = false;
    overlays.celestialEquator = false;
    overlays.circumpolarBoundary = false;
    return overlays;
}

int childCount(const QSGNode& node)
{
    int count = 0;
    for (const QSGNode* child = node.firstChild(); child != nullptr; child = child->nextSibling()) {
        ++count;
    }
    return count;
}

const QSGGeometryNode* firstGeometryChild(const QSGNode& node)
{
    const QSGNode* child = node.firstChild();
    if (child == nullptr || child->type() != QSGNode::GeometryNodeType) {
        return nullptr;
    }
    return static_cast<const QSGGeometryNode*>(child);
}

}  // namespace

class SkyViewportInternalsTests final : public QObject {
    Q_OBJECT

private slots:
    void geometryAppendsFrameLinesAndDeepSkyGlyphSegments();
    void sceneGraphBucketsLineSegmentsAndSkipsDegenerateLines();
    void sceneGraphBucketsPointsByColor();
    void sceneRootOwnsSeparateLineAndPointRoots();
};

void SkyViewportInternalsTests::geometryAppendsFrameLinesAndDeepSkyGlyphSegments()
{
    const auto projection = makeProjection();
    QVERIFY(projection.has_value());

    skygate::ui::internal::SkyThemeRenderPalette theme;
    theme.gridAltitudeLine = QColor("#111111");
    theme.gridAzimuthLine = QColor("#222222");
    theme.horizonLine = QColor("#333333");
    theme.eclipticLine = QColor("#444444");
    theme.celestialEquatorLine = QColor("#555555");
    theme.circumpolarBoundaryLine = QColor("#666666");

    const std::vector<SkyRenderLine> frameLines {
        {
            .x1 = 10.0,
            .y1 = 20.0,
            .x2 = 30.0,
            .y2 = 45.0,
            .widthPx = 2.5,
            .color = QColor("#abcdef")
        }
    };
    const std::vector<SkyRenderGlyph> glyphs {
        {
            .x = 100.0,
            .y = 120.0,
            .radiusXPx = 8.0,
            .radiusYPx = 6.0,
            .rotationDeg = 0.0,
            .widthPx = 1.5,
            .kind = skygate::ephemeris::DeepSkyObjectKind::GlobularCluster,
            .color = QColor("#fedcba")
        }
    };

    const auto segments = skygate::ui::internal::buildSkyViewportLineSegments(
        skygate::ui::internal::SkyViewportGeometryInput {
            .projection = *projection,
            .viewportWidth = 800.0,
            .viewportHeight = 600.0,
            .renderTheme = theme,
            .overlayLayers = hiddenOverlays(),
            .observer = {},
            .utcTime = {},
            .renderLines = frameLines,
            .renderGlyphs = glyphs
        }
    );

    QCOMPARE(segments.size(), 35U);
    QCOMPARE(segments.front().x1, 10.0F);
    QCOMPARE(segments.front().y1, 20.0F);
    QCOMPARE(segments.front().x2, 30.0F);
    QCOMPARE(segments.front().y2, 45.0F);
    QCOMPARE(segments.front().widthPx, 2.5F);
    QCOMPARE(segments.front().color, QColor("#abcdef"));

    const auto glyphSegmentCount = std::count_if(
        segments.begin(),
        segments.end(),
        [](const skygate::ui::internal::SkyViewportLineSegment& segment) {
            return segment.color == QColor("#fedcba");
        }
    );
    QCOMPARE(glyphSegmentCount, 34);
}

void SkyViewportInternalsTests::sceneGraphBucketsLineSegmentsAndSkipsDegenerateLines()
{
    QSGNode root;
    const std::vector<skygate::ui::internal::SkyViewportLineSegment> segments {
        {
            .x1 = 0.0F,
            .y1 = 0.0F,
            .x2 = 10.0F,
            .y2 = 0.0F,
            .widthPx = 2.0F,
            .color = QColor("#ff0000")
        },
        {
            .x1 = 4.0F,
            .y1 = 4.0F,
            .x2 = 4.0F,
            .y2 = 12.0F,
            .widthPx = 2.0F,
            .color = QColor("#ff0000")
        },
        {
            .x1 = 5.0F,
            .y1 = 5.0F,
            .x2 = 5.0F,
            .y2 = 5.0F,
            .widthPx = 2.0F,
            .color = QColor("#ff0000")
        },
    };

    skygate::ui::internal::syncSkyViewportLineNodes(&root, segments);

    QCOMPARE(childCount(root), 1);
    const QSGGeometryNode* geometryNode = firstGeometryChild(root);
    QVERIFY(geometryNode != nullptr);
    QCOMPARE(geometryNode->geometry()->vertexCount(), 12);
    QCOMPARE(
        geometryNode->geometry()->drawingMode(),
        static_cast<unsigned int>(QSGGeometry::DrawTriangles)
    );
    QCOMPARE(
        static_cast<QSGFlatColorMaterial*>(geometryNode->material())->color(),
        QColor("#ff0000")
    );

    skygate::ui::internal::clearSkyViewportChildNodes(&root);
    QCOMPARE(childCount(root), 0);
}

void SkyViewportInternalsTests::sceneGraphBucketsPointsByColor()
{
    QSGNode root;
    const std::vector<SkyRenderPoint> points {
        {.x = 10.0, .y = 20.0, .sizePx = 4.0, .color = QColor("#00ff00")},
        {.x = 30.0, .y = 40.0, .sizePx = 6.0, .color = QColor("#00ff00")},
        {.x = 50.0, .y = 60.0, .sizePx = 8.0, .color = QColor("#0000ff")},
    };

    skygate::ui::internal::syncSkyViewportPointNodes(&root, points);

    QCOMPARE(childCount(root), 2);
    int totalVertices = 0;
    for (const QSGNode* child = root.firstChild(); child != nullptr; child = child->nextSibling()) {
        QVERIFY(child->type() == QSGNode::GeometryNodeType);
        totalVertices += static_cast<const QSGGeometryNode*>(child)->geometry()->vertexCount();
    }
    QCOMPARE(totalVertices, 18);
}

void SkyViewportInternalsTests::sceneRootOwnsSeparateLineAndPointRoots()
{
    skygate::ui::internal::SkyViewportSceneRootNode root;

    QVERIFY(root.lineRoot() != nullptr);
    QVERIFY(root.pointRoot() != nullptr);
    QVERIFY(root.lineRoot() != root.pointRoot());
    QCOMPARE(root.firstChild(), root.lineRoot());
    QCOMPARE(root.lineRoot()->nextSibling(), root.pointRoot());
}

QTEST_APPLESS_MAIN(SkyViewportInternalsTests)

#include "SkyViewportInternalsTests.moc"
