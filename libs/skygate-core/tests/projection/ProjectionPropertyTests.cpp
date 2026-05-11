#include "skygate/core/PreparedProjection.hpp"
#include "skygate/core/ProjectionFactory.hpp"
#include "skygate/testsupport/DeterministicFuzz.hpp"

#include <QtTest/QtTest>

#include <algorithm>
#include <array>
#include <cmath>

namespace {

constexpr std::array<skygate::core::ProjectionType, 3> kProjectionTypes {
    skygate::core::ProjectionType::Stereographic,
    skygate::core::ProjectionType::AzimuthalEquidistant,
    skygate::core::ProjectionType::Perspective,
};

[[nodiscard]] bool isNear(
    const double actual,
    const double expected,
    const double tolerance = 1e-6
)
{
    return std::abs(actual - expected) <= tolerance;
}

[[nodiscard]] skygate::core::ProjectionParams generatedParams(
    skygate::testsupport::DeterministicRng& rng
)
{
    return skygate::core::ProjectionParams {
        .center = {
            .altitudeDeg = rng.realInRange(-75.0, 75.0),
            .azimuthDeg = rng.realInRange(-720.0, 720.0),
        },
        .fovDeg = rng.realInRange(25.0, 135.0),
        .rollDeg = rng.realInRange(-720.0, 720.0),
        .viewportWidth = rng.realInRange(240.0, 2600.0),
        .viewportHeight = rng.realInRange(180.0, 1800.0),
    };
}

[[nodiscard]] skygate::core::HorizontalCoordinate generatedCoordinateNear(
    skygate::testsupport::DeterministicRng& rng,
    const skygate::core::HorizontalCoordinate& center,
    const double radiusDeg
)
{
    return skygate::core::HorizontalCoordinate {
        .altitudeDeg = std::clamp(
            center.altitudeDeg + rng.realInRange(-radiusDeg, radiusDeg),
            -90.0,
            90.0
        ),
        .azimuthDeg = center.azimuthDeg + rng.realInRange(-radiusDeg, radiusDeg),
    };
}

void verifyEquivalentPoints(
    const skygate::core::ScreenPoint& actual,
    const skygate::core::ScreenPoint& expected
)
{
    QCOMPARE(actual.status, expected.status);
    QCOMPARE(actual.isVisible, expected.isVisible);
    if (actual.status == skygate::core::ProjectionStatus::Visible) {
        QVERIFY(actual.isFinite());
        QVERIFY(expected.isFinite());
        QVERIFY(isNear(actual.x, expected.x));
        QVERIFY(isNear(actual.y, expected.y));
    }
}

}  // namespace

class ProjectionPropertyTests final : public QObject {
    Q_OBJECT

private slots:
    void generatedPreparedProjectionsMatchDirectProjection();
    void generatedVisiblePointsAreFiniteAndWithinViewport();
    void addingFullAzimuthTurnsDoesNotChangeProjection();
    void invalidGeneratedCoordinatesNeverBecomeVisible();
};

void ProjectionPropertyTests::generatedPreparedProjectionsMatchDirectProjection()
{
    skygate::testsupport::DeterministicRng rng(0x9a700001U);

    for (const auto projectionType : kProjectionTypes) {
        const auto projection = skygate::core::createProjection(projectionType);
        QVERIFY(projection != nullptr);

        for (int sample = 0; sample < 180; ++sample) {
            const auto params = generatedParams(rng);
            const auto preparedProjection =
                skygate::core::PreparedProjection::create(projectionType, params);
            QVERIFY(preparedProjection.has_value());

            for (int coordinateIndex = 0; coordinateIndex < 6; ++coordinateIndex) {
                const auto coordinate = generatedCoordinateNear(rng, params.center, params.fovDeg);
                verifyEquivalentPoints(
                    preparedProjection->project(coordinate),
                    projection->project(coordinate, params)
                );
            }
        }
    }
}

void ProjectionPropertyTests::generatedVisiblePointsAreFiniteAndWithinViewport()
{
    skygate::testsupport::DeterministicRng rng(0x9a700002U);

    for (const auto projectionType : kProjectionTypes) {
        const auto projection = skygate::core::createProjection(projectionType);
        QVERIFY(projection != nullptr);

        for (int sample = 0; sample < 240; ++sample) {
            const auto params = generatedParams(rng);
            const auto coordinate = generatedCoordinateNear(rng, params.center, params.fovDeg * 0.45);
            const auto point = projection->project(coordinate, params);

            if (point.status == skygate::core::ProjectionStatus::Visible) {
                QVERIFY(point.isVisible);
                QVERIFY(point.isFinite());
                QVERIFY(point.x >= -1e-6);
                QVERIFY(point.x <= params.viewportWidth + 1e-6);
                QVERIFY(point.y >= -1e-6);
                QVERIFY(point.y <= params.viewportHeight + 1e-6);
            } else {
                QVERIFY(!point.isVisible);
            }
        }
    }
}

void ProjectionPropertyTests::addingFullAzimuthTurnsDoesNotChangeProjection()
{
    skygate::testsupport::DeterministicRng rng(0x9a700003U);

    for (const auto projectionType : kProjectionTypes) {
        const auto projection = skygate::core::createProjection(projectionType);
        QVERIFY(projection != nullptr);

        for (int sample = 0; sample < 180; ++sample) {
            auto params = generatedParams(rng);
            auto shiftedParams = params;
            shiftedParams.center.azimuthDeg += 360.0 * static_cast<double>(rng.intInRange(-4, 4));

            auto coordinate = generatedCoordinateNear(rng, params.center, params.fovDeg * 0.5);
            auto shiftedCoordinate = coordinate;
            shiftedCoordinate.azimuthDeg += 360.0 * static_cast<double>(rng.intInRange(-4, 4));

            verifyEquivalentPoints(
                projection->project(shiftedCoordinate, shiftedParams),
                projection->project(coordinate, params)
            );
        }
    }
}

void ProjectionPropertyTests::invalidGeneratedCoordinatesNeverBecomeVisible()
{
    skygate::testsupport::DeterministicRng rng(0x9a700004U);

    for (const auto projectionType : kProjectionTypes) {
        const auto projection = skygate::core::createProjection(projectionType);
        QVERIFY(projection != nullptr);

        for (int sample = 0; sample < 96; ++sample) {
            const auto params = generatedParams(rng);
            const skygate::core::HorizontalCoordinate invalidCoordinate {
                .altitudeDeg = rng.chance(1U, 2U)
                    ? rng.realInRange(90.01, 720.0)
                    : rng.realInRange(-720.0, -90.01),
                .azimuthDeg = rng.realInRange(-720.0, 720.0),
            };
            const auto point = projection->project(invalidCoordinate, params);

            QCOMPARE(point.status, skygate::core::ProjectionStatus::InvalidCoordinate);
            QVERIFY(!point.isVisible);
        }
    }
}

QTEST_APPLESS_MAIN(ProjectionPropertyTests)

#include "ProjectionPropertyTests.moc"
