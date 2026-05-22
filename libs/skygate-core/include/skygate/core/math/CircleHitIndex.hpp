#pragma once

#include "skygate/core/math/CircleHitTarget.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace skygate::core {

class CircleHitIndex final {
public:
    explicit CircleHitIndex(double cellSize);

    void rebuild(std::span<const CircleHitTarget> targets);
    void clear();
    [[nodiscard]] std::optional<std::uint32_t> nearestPayloadAt(double x, double y) const;

private:
    double m_cellSize = 24.0;
    std::vector<CircleHitTarget> m_targets;
    std::unordered_map<std::uint64_t, std::vector<std::size_t>> m_targetIndicesByCell;
};

}  // namespace skygate::core
