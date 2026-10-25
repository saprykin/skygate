#include "projections/ProjectionAlgorithms.hpp"
#include "projections/ProjectionTypeCodec.hpp"

#include <QtTest/QtTest>

#include <array>
#include <string_view>

namespace {

constexpr std::array<skygate::core::ProjectionType, 2> kCircularProjectionTypes{
    skygate::core::ProjectionType::Stereographic,
    skygate::core::ProjectionType::AzimuthalEquidistant,
};

[[nodiscard]] QString projectionTypeText(const skygate::core::ProjectionType projectionType)
{
    const std::string_view text = skygate::core::ProjectionTypeCodec::text(projectionType);
    return QString::fromLatin1(text.data(), static_cast<qsizetype>(text.size()));
}

[[nodiscard]] skygate::core::ScreenPoint projectPoint(
    const skygate::core::ProjectionType projectionType,
    const skygate::core::HorizontalCoordinate& coordinate,
    const skygate::core::ProjectionParams& params
)
{
    skygate::core::ProjectionFrame frame;
    if (!skygate::core::ProjectionAlgorithms::prepareFrame(projectionType, params, frame)) {
        return skygate::core::ScreenPoint::invalidParameters();
    }

    return skygate::core::ProjectionAlgorithms::project(frame, coordinate);
}

}  // namespace

class CircularProjectionCullingTests final : public QObject {
    Q_OBJECT

private slots:
    void oppositeDirectionIsCulled();
    void directionAtCircularFovEdgeIsVisible();
    void directionOutsideCircularFovIsCulled();
};

void CircularProjectionCullingTests::oppositeDirectionIsCulled()
{
    const skygate::core::ProjectionParams params{
        .center = {.altitudeDeg = 45.0, .azimuthDeg = 180.0},
        .fovDeg = 90.0,
        .rollDeg = 0.0,
        .viewportWidth = 1100.0,
        .viewportHeight = 760.0,
    };

    for (const auto projectionType : kCircularProjectionTypes) {
        const auto oppositePoint = projectPoint(projectionType, {.altitudeDeg = -45.0, .azimuthDeg = 0.0}, params);

        QVERIFY2(!oppositePoint.isVisible(), qPrintable(projectionTypeText(projectionType)));
        QVERIFY2(
            oppositePoint.status == skygate::core::ProjectionStatus::Culled,
            qPrintable(projectionTypeText(projectionType))
        );
    }
}

void CircularProjectionCullingTests::directionAtCircularFovEdgeIsVisible()
{
    const skygate::core::ProjectionParams params{
        .center = {.altitudeDeg = 0.0, .azimuthDeg = 0.0},
        .fovDeg = 60.0,
        .rollDeg = 0.0,
        .viewportWidth = 1000.0,
        .viewportHeight = 1000.0,
    };

    for (const auto projectionType : kCircularProjectionTypes) {
        const auto edgePoint = projectPoint(projectionType, {.altitudeDeg = 0.0, .azimuthDeg = 30.0}, params);
        QVERIFY2(edgePoint.isVisible(), qPrintable(projectionTypeText(projectionType)));
        QVERIFY2(
            edgePoint.status == skygate::core::ProjectionStatus::Visible, qPrintable(projectionTypeText(projectionType))
        );

        const auto outsidePoint = projectPoint(projectionType, {.altitudeDeg = 0.0, .azimuthDeg = 30.25}, params);
        QVERIFY2(!outsidePoint.isVisible(), qPrintable(projectionTypeText(projectionType)));
        QVERIFY2(
            outsidePoint.status == skygate::core::ProjectionStatus::Culled,
            qPrintable(projectionTypeText(projectionType))
        );
    }
}

void CircularProjectionCullingTests::directionOutsideCircularFovIsCulled()
{
    const skygate::core::ProjectionParams params{
        .center = {.altitudeDeg = 0.0, .azimuthDeg = 0.0},
        .fovDeg = 20.0,
        .rollDeg = 0.0,
        .viewportWidth = 1000.0,
        .viewportHeight = 1000.0,
    };

    for (const auto projectionType : kCircularProjectionTypes) {
        const auto outsideFovPoint = projectPoint(projectionType, {.altitudeDeg = 0.0, .azimuthDeg = 50.0}, params);
        QVERIFY2(!outsideFovPoint.isVisible(), qPrintable(projectionTypeText(projectionType)));
        QVERIFY2(
            outsideFovPoint.status == skygate::core::ProjectionStatus::Culled,
            qPrintable(projectionTypeText(projectionType))
        );
    }
}

QTEST_APPLESS_MAIN(CircularProjectionCullingTests)

#include "CircularProjectionCullingTests.moc"
