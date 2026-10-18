#pragma once

#include "ProjectionType.hpp"

#include <optional>
#include <string_view>
#include <vector>

namespace skygate::core {

// Canonical text and ordering for projection names. Every consumer that
// renders or parses a projection name uses this mapping so the display order
// and the accepted spellings live in one place.
class ProjectionTypeCodec final {
public:
    [[nodiscard]] static std::vector<ProjectionType> orderedValues();
    [[nodiscard]] static std::string_view text(ProjectionType projectionType) noexcept;
    [[nodiscard]] static std::optional<ProjectionType> fromText(std::string_view text) noexcept;
};

}  // namespace skygate::core
