#include "skygate/core/ProjectionFactory.hpp"

#include <algorithm>
#include <cmath>

namespace skygate::core {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegreesToRadians = kPi / 180.0;
constexpr double kEpsilon = 1e-12;

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

[[nodiscard]] double dot(const Vec3& lhs, const Vec3& rhs) noexcept
{
    return (lhs.x * rhs.x) + (lhs.y * rhs.y) + (lhs.z * rhs.z);
}

[[nodiscard]] Vec3 cross(const Vec3& lhs, const Vec3& rhs) noexcept
{
    return {
        (lhs.y * rhs.z) - (lhs.z * rhs.y),
        (lhs.z * rhs.x) - (lhs.x * rhs.z),
        (lhs.x * rhs.y) - (lhs.y * rhs.x)
    };
}

[[nodiscard]] double length(const Vec3& vector) noexcept
{
    return std::sqrt(dot(vector, vector));
}

[[nodiscard]] Vec3 normalize(const Vec3& vector) noexcept
{
    const double vectorLength = length(vector);
    if (vectorLength <= kEpsilon) {
        return {};
    }

    return {
        vector.x / vectorLength,
        vector.y / vectorLength,
        vector.z / vectorLength
    };
}

[[nodiscard]] double toRadians(double degrees) noexcept
{
    return degrees * kDegreesToRadians;
}

[[nodiscard]] bool isFinite(double value) noexcept
{
    return std::isfinite(value);
}

[[nodiscard]] Vec3 horizontalToUnitVector(const HorizontalCoordinate& coordinate) noexcept
{
    const double altitudeRad = toRadians(coordinate.altitudeDeg);
    const double azimuthRad = toRadians(coordinate.azimuthDeg);

    const double cosAltitude = std::cos(altitudeRad);
    return {
        cosAltitude * std::sin(azimuthRad),
        cosAltitude * std::cos(azimuthRad),
        std::sin(altitudeRad)
    };
}

class StereographicProjection final : public IProjection {
public:
    [[nodiscard]] ProjectionType type() const noexcept override
    {
        return ProjectionType::Stereographic;
    }

    [[nodiscard]] ScreenPoint project(
        const HorizontalCoordinate& coordinate,
        const ProjectionParams& params
    ) const noexcept override
    {
        ScreenPoint point;

        if (!isFinite(params.fovDeg) || !isFinite(params.rollDeg) ||
            !isFinite(params.viewportWidth) || !isFinite(params.viewportHeight) ||
            params.viewportWidth <= 0.0 || params.viewportHeight <= 0.0 ||
            params.fovDeg <= 0.0 || params.fovDeg >= 179.0) {
            return point;
        }

        const Vec3 center = horizontalToUnitVector(params.center);
        const Vec3 target = horizontalToUnitVector(coordinate);

        constexpr Vec3 zenithAxis {0.0, 0.0, 1.0};
        constexpr Vec3 northAxis {0.0, 1.0, 0.0};

        Vec3 right = cross(zenithAxis, center);
        if (length(right) <= kEpsilon) {
            right = cross(northAxis, center);
        }
        right = normalize(right);
        if (length(right) <= kEpsilon) {
            return point;
        }

        const Vec3 up = normalize(cross(center, right));
        if (length(up) <= kEpsilon) {
            return point;
        }

        const double centerDotTarget = std::clamp(dot(target, center), -1.0, 1.0);
        const double denominator = 1.0 + centerDotTarget;
        if (denominator <= kEpsilon) {
            return point;
        }

        double projectedX = 2.0 * dot(target, right) / denominator;
        double projectedY = 2.0 * dot(target, up) / denominator;

        const double rollRad = toRadians(params.rollDeg);
        const double cosRoll = std::cos(rollRad);
        const double sinRoll = std::sin(rollRad);
        const double rotatedX = (projectedX * cosRoll) - (projectedY * sinRoll);
        const double rotatedY = (projectedX * sinRoll) + (projectedY * cosRoll);

        const double halfFovRad = toRadians(params.fovDeg) * 0.5;
        const double maxRadius = 2.0 * std::tan(halfFovRad * 0.5);
        if (maxRadius <= kEpsilon || !isFinite(maxRadius)) {
            return point;
        }

        const double radius = std::sqrt((rotatedX * rotatedX) + (rotatedY * rotatedY));
        if (radius > maxRadius) {
            return point;
        }

        const double scale = (0.5 * std::min(params.viewportWidth, params.viewportHeight)) / maxRadius;
        point.x = (0.5 * params.viewportWidth) + (rotatedX * scale);
        point.y = (0.5 * params.viewportHeight) - (rotatedY * scale);
        point.isVisible = isFinite(point.x) && isFinite(point.y);
        return point;
    }
};

}  // namespace

std::unique_ptr<IProjection> createProjection(const ProjectionType type)
{
    switch (type) {
    case ProjectionType::Stereographic:
        return std::make_unique<StereographicProjection>();
    case ProjectionType::AzimuthalEquidistant:
        return nullptr;
    }

    return nullptr;
}

}  // namespace skygate::core
