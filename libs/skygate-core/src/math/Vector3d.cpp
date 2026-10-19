#include "Vector3d.hpp"

#include <cmath>
#include <limits>

namespace skygate::core {

bool Vector3d::isFinite() const noexcept
{
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
}

double Vector3d::length() const noexcept
{
    return std::hypot(std::hypot(x, y), z);
}

std::optional<Vector3d> Vector3d::normalized() const noexcept
{
    const double vectorLength = length();
    // The smallest normal double is deliberate: below it a vector has no
    // usable direction, and kEpsilon is a different kind of threshold.
    if (!std::isfinite(vectorLength) || vectorLength <= std::numeric_limits<double>::min()) {
        return std::nullopt;
    }

    return scaled(1.0 / vectorLength);
}

std::optional<Vector3d> Vector3d::withLength(const double targetLength) const noexcept
{
    const std::optional<Vector3d> direction = normalized();
    // Same bound as normalized(): a non-finite or subnormal target length
    // carries no usable magnitude.
    if (!std::isfinite(targetLength) || targetLength <= std::numeric_limits<double>::min() || !direction.has_value()) {
        return std::nullopt;
    }

    return direction->scaled(targetLength);
}

Vector3d Vector3d::scaled(const double scale) const noexcept
{
    return {
        .x = x * scale,
        .y = y * scale,
        .z = z * scale,
    };
}

double Vector3d::dot(const Vector3d& other) const noexcept
{
    return (x * other.x) + (y * other.y) + (z * other.z);
}

Vector3d Vector3d::cross(const Vector3d& other) const noexcept
{
    return {
        .x = (y * other.z) - (z * other.y),
        .y = (z * other.x) - (x * other.z),
        .z = (x * other.y) - (y * other.x),
    };
}

Vector3d Vector3d::operator+(const Vector3d& other) const noexcept
{
    return {
        .x = x + other.x,
        .y = y + other.y,
        .z = z + other.z,
    };
}

Vector3d Vector3d::operator-(const Vector3d& other) const noexcept
{
    return {
        .x = x - other.x,
        .y = y - other.y,
        .z = z - other.z,
    };
}

Vector3d Vector3d::operator*(const double scale) const noexcept
{
    return scaled(scale);
}

Vector3d& Vector3d::operator+=(const Vector3d& other) noexcept
{
    x += other.x;
    y += other.y;
    z += other.z;
    return *this;
}

Vector3d& Vector3d::operator-=(const Vector3d& other) noexcept
{
    x -= other.x;
    y -= other.y;
    z -= other.z;
    return *this;
}

Vector3d& Vector3d::operator*=(const double scale) noexcept
{
    x *= scale;
    y *= scale;
    z *= scale;
    return *this;
}

}  // namespace skygate::core
