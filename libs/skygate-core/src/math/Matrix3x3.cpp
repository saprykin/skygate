#include "Matrix3x3.hpp"

#include <cassert>

namespace skygate::core {
namespace {

constexpr std::size_t kDimension = 3U;

[[nodiscard]] std::size_t valueIndex(const std::size_t rowIndex, const std::size_t columnIndex) noexcept
{
    assert(rowIndex < kDimension);
    assert(columnIndex < kDimension);
    return (rowIndex * kDimension) + columnIndex;
}

}  // namespace

Matrix3x3::Matrix3x3(const Vector3d& firstRow, const Vector3d& secondRow, const Vector3d& thirdRow) noexcept
    : m_values{
          firstRow.x,
          firstRow.y,
          firstRow.z,
          secondRow.x,
          secondRow.y,
          secondRow.z,
          thirdRow.x,
          thirdRow.y,
          thirdRow.z,
      }
{
}

Matrix3x3 Matrix3x3::identity() noexcept
{
    return {
        {.x = 1.0, .y = 0.0, .z = 0.0},
        {.x = 0.0, .y = 1.0, .z = 0.0},
        {.x = 0.0, .y = 0.0, .z = 1.0},
    };
}

Vector3d Matrix3x3::row(const std::size_t rowIndex) const noexcept
{
    assert(rowIndex < kDimension);
    const std::size_t offset = rowIndex * kDimension;
    return {
        .x = m_values[offset],
        .y = m_values[offset + 1U],
        .z = m_values[offset + 2U],
    };
}

double Matrix3x3::value(const std::size_t rowIndex, const std::size_t columnIndex) const noexcept
{
    return m_values[valueIndex(rowIndex, columnIndex)];
}

Vector3d Matrix3x3::multiplied(const Vector3d& vector) const noexcept
{
    return {
        .x = value(0U, 0U) * vector.x + value(0U, 1U) * vector.y + value(0U, 2U) * vector.z,
        .y = value(1U, 0U) * vector.x + value(1U, 1U) * vector.y + value(1U, 2U) * vector.z,
        .z = value(2U, 0U) * vector.x + value(2U, 1U) * vector.y + value(2U, 2U) * vector.z,
    };
}

Vector3d Matrix3x3::transposeMultiplied(const Vector3d& vector) const noexcept
{
    return {
        .x = value(0U, 0U) * vector.x + value(1U, 0U) * vector.y + value(2U, 0U) * vector.z,
        .y = value(0U, 1U) * vector.x + value(1U, 1U) * vector.y + value(2U, 1U) * vector.z,
        .z = value(0U, 2U) * vector.x + value(1U, 2U) * vector.y + value(2U, 2U) * vector.z,
    };
}

Matrix3x3 Matrix3x3::transposed() const noexcept
{
    return {
        {.x = value(0U, 0U), .y = value(1U, 0U), .z = value(2U, 0U)},
        {.x = value(0U, 1U), .y = value(1U, 1U), .z = value(2U, 1U)},
        {.x = value(0U, 2U), .y = value(1U, 2U), .z = value(2U, 2U)},
    };
}

}  // namespace skygate::core
