#pragma once

#include "SkySelectionOverlayInput.hpp"

#include <QString>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

class SkyTrailTargetResolver final {
public:
    // One target of the overlay. The trail and the selection marker read the
    // same candidate list, so the target precedence lives in one place; each
    // caller keeps the kind filter it needs.
    struct Target final {
        QString kind;
        QString id;
    };

    static constexpr std::size_t kTargetCount = 3;

    // The candidates in the order the overlay follows them: the selected
    // object, the tracked target, then the selected search target.
    [[nodiscard]] std::array<Target, kTargetCount> targetCandidates(const SkySelectionOverlayInput& input) const;

    [[nodiscard]] QString activeTrailTargetBodyId(const SkySelectionOverlayInput& input) const;
    [[nodiscard]] std::optional<std::uint32_t> activeTrailTargetBodyIndex(const SkySelectionOverlayInput& input) const;
};
