#pragma once

#include <optional>

namespace skygate::core {

struct Vector3d final {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    [[nodiscard]] bool isFinite() const noexcept;
    [[nodiscard]] double length() const noexcept;
    [[nodiscard]] std::optional<Vector3d> normalized() const noexcept;
    [[nodiscard]] Vector3d scaled(double scale) const noexcept;
    [[nodiscard]] double dot(const Vector3d& other) const noexcept;
    [[nodiscard]] Vector3d cross(const Vector3d& other) const noexcept;

    [[nodiscard]] Vector3d operator+(const Vector3d& other) const noexcept;
    [[nodiscard]] Vector3d operator-(const Vector3d& other) const noexcept;
    [[nodiscard]] Vector3d operator*(double scale) const noexcept;
    Vector3d& operator+=(const Vector3d& other) noexcept;
    Vector3d& operator-=(const Vector3d& other) noexcept;
    Vector3d& operator*=(double scale) noexcept;
};

}  // namespace skygate::core
