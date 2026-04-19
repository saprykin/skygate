#include "skygate/core/PreparedProjection.hpp"
#include "skygate/core/ProjectionFactory.hpp"

#include <QtTest/QtTest>

#include <array>
#include <cmath>

namespace {

[[nodiscard]] bool isNear(const double value, const double expected, const double tolerance)
{
    return std::abs(value - expected) <= tolerance;
}

}  // namespace

class PreparedProjectionTests final : public QObject {
    Q_OBJECT

private slots:
    void matchesDirectProjectionForAllProjectionTypes();
};

void PreparedProjectionTests::matchesDirectProjectionForAllProjectionTypes()
{
    const std::array<skygate::core::ProjectionType, 3> projectionTypes {
        skygate::core::ProjectionType::Stereographic,
        skygate::core::ProjectionType::AzimuthalEquidistant,
        skygate::core::ProjectionType::Perspective
    };
    const std::array<skygate::core::HorizontalCoordinate, 5> sampleCoordinates {{
        {.altitudeDeg = 45.0, .azimuthDeg = 180.0},
        {.altitudeDeg = 60.0, .azimuthDeg = 150.0},
        {.altitudeDeg = 25.0, .azimuthDeg = 210.0},
        {.altitudeDeg = 5.0, .azimuthDeg = 90.0},
        {.altitudeDeg = -15.0, .azimuthDeg = 310.0},
    }};

    const skygate::core::ProjectionParams params {
        .center = {.altitudeDeg = 45.0, .azimuthDeg = 180.0},
        .fovDeg = 100.0,
        .rollDeg = 12.5,
        .viewportWidth = 1280.0,
        .viewportHeight = 720.0,
    };

    for (const auto projectionType : projectionTypes) {
        const auto projection = skygate::core::createProjection(projectionType);
        QVERIFY(projection != nullptr);

        const auto preparedProjection =
            skygate::core::PreparedProjection::create(projectionType, params);
        QVERIFY(preparedProjection.has_value());

        for (const auto& coordinate : sampleCoordinates) {
            const auto directPoint = projection->project(coordinate, params);
            const auto preparedPoint = preparedProjection->project(coordinate);

            QCOMPARE(preparedPoint.status, directPoint.status);
            QCOMPARE(preparedPoint.isVisible, directPoint.isVisible);
            QVERIFY(isNear(preparedPoint.x, directPoint.x, 1e-6));
            QVERIFY(isNear(preparedPoint.y, directPoint.y, 1e-6));
        }
    }
}

QTEST_APPLESS_MAIN(PreparedProjectionTests)

#include "PreparedProjectionTests.moc"
