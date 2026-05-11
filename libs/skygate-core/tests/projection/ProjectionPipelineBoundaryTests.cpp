#include "../../src/projections/ProjectionPipeline.hpp"

#include <QtTest/QtTest>

#include <cmath>
#include <limits>

namespace {

[[nodiscard]] skygate::core::ProjectionParams baseParams()
{
    return skygate::core::ProjectionParams {
        .center = {.altitudeDeg = 0.0, .azimuthDeg = 0.0},
        .fovDeg = 90.0,
        .rollDeg = 0.0,
        .viewportWidth = 800.0,
        .viewportHeight = 600.0,
    };
}

void expectInvalidParameters(const skygate::core::ScreenPoint& point)
{
    QCOMPARE(point.status, skygate::core::ProjectionStatus::InvalidParameters);
    QVERIFY(!point.isVisible);
}

void expectCulled(const skygate::core::ScreenPoint& point)
{
    QCOMPARE(point.status, skygate::core::ProjectionStatus::Culled);
    QVERIFY(!point.isVisible);
}

}  // namespace

class ProjectionPipelineBoundaryTests final : public QObject {
    Q_OBJECT

private slots:
    void finishCircularRejectsInvalidScaleInputs();
    void finishCircularCullsOutsideRadiusAndAllowsVisiblePoints();
    void finishRectangularRejectsInvalidScaleInputs();
    void finishRectangularCullsOutsideBoundsAndAllowsVisiblePoints();
};

void ProjectionPipelineBoundaryTests::finishCircularRejectsInvalidScaleInputs()
{
    using skygate::core::ProjectionPipeline;

    auto params = baseParams();
    expectInvalidParameters(ProjectionPipeline::finishCircular(0.0, 0.0, params, 0.0));
    expectInvalidParameters(ProjectionPipeline::finishCircular(
        0.0,
        0.0,
        params,
        std::numeric_limits<double>::quiet_NaN()
    ));

    params.viewportWidth = 0.0;
    params.viewportHeight = 0.0;
    expectInvalidParameters(ProjectionPipeline::finishCircular(0.0, 0.0, params, 1.0));

    params = baseParams();
    expectInvalidParameters(ProjectionPipeline::finishCircular(
        std::numeric_limits<double>::quiet_NaN(),
        0.0,
        params,
        1.0
    ));
}

void ProjectionPipelineBoundaryTests::finishCircularCullsOutsideRadiusAndAllowsVisiblePoints()
{
    using skygate::core::ProjectionPipeline;

    const auto params = baseParams();
    const auto visible = ProjectionPipeline::finishCircular(0.5, -0.25, params, 1.0);
    QCOMPARE(visible.status, skygate::core::ProjectionStatus::Visible);
    QVERIFY(visible.isVisible);
    QVERIFY(std::isfinite(visible.x));
    QVERIFY(std::isfinite(visible.y));

    expectCulled(ProjectionPipeline::finishCircular(1.01, 0.0, params, 1.0, -20.0));
    const auto visibleWithMargin = ProjectionPipeline::finishCircular(1.02, 0.0, params, 1.0, 16.0);
    QCOMPARE(visibleWithMargin.status, skygate::core::ProjectionStatus::Visible);
}

void ProjectionPipelineBoundaryTests::finishRectangularRejectsInvalidScaleInputs()
{
    using skygate::core::ProjectionPipeline;

    auto params = baseParams();
    expectInvalidParameters(ProjectionPipeline::finishRectangular(0.0, 0.0, params, 0.0, 1.0));
    expectInvalidParameters(ProjectionPipeline::finishRectangular(0.0, 0.0, params, 1.0, 0.0));
    expectInvalidParameters(ProjectionPipeline::finishRectangular(
        0.0,
        0.0,
        params,
        std::numeric_limits<double>::infinity(),
        1.0
    ));

    params.viewportWidth = 0.0;
    expectInvalidParameters(ProjectionPipeline::finishRectangular(0.0, 0.0, params, 1.0, 1.0));

    params = baseParams();
    expectInvalidParameters(ProjectionPipeline::finishRectangular(
        std::numeric_limits<double>::quiet_NaN(),
        0.0,
        params,
        1.0,
        1.0
    ));
}

void ProjectionPipelineBoundaryTests::finishRectangularCullsOutsideBoundsAndAllowsVisiblePoints()
{
    using skygate::core::ProjectionPipeline;

    const auto params = baseParams();
    const auto visible = ProjectionPipeline::finishRectangular(0.5, -0.5, params, 1.0, 1.0);
    QCOMPARE(visible.status, skygate::core::ProjectionStatus::Visible);
    QVERIFY(visible.isVisible);
    QVERIFY(std::isfinite(visible.x));
    QVERIFY(std::isfinite(visible.y));

    expectCulled(ProjectionPipeline::finishRectangular(1.01, 0.0, params, 1.0, 1.0, -20.0));
    const auto visibleWithMargin =
        ProjectionPipeline::finishRectangular(1.01, 0.0, params, 1.0, 1.0, 10.0);
    QCOMPARE(visibleWithMargin.status, skygate::core::ProjectionStatus::Visible);
}

QTEST_APPLESS_MAIN(ProjectionPipelineBoundaryTests)

#include "ProjectionPipelineBoundaryTests.moc"
