#pragma once

#include "Vector3d.hpp"

#include <array>
#include <cstddef>

namespace skygate::core {

class Matrix3x3 final {
public:
    Matrix3x3() noexcept = default;
    Matrix3x3(const Vector3d& firstRow, const Vector3d& secondRow, const Vector3d& thirdRow) noexcept;

    [[nodiscard]] static Matrix3x3 identity() noexcept;

    [[nodiscard]] Vector3d row(std::size_t rowIndex) const noexcept;
    [[nodiscard]] double value(std::size_t rowIndex, std::size_t columnIndex) const noexcept;
    [[nodiscard]] Vector3d multiplied(const Vector3d& vector) const noexcept;
    [[nodiscard]] Vector3d transposeMultiplied(const Vector3d& vector) const noexcept;
    [[nodiscard]] Matrix3x3 transposed() const noexcept;

private:
    std::array<double, 9> m_values{};
};

}  // namespace skygate::core
