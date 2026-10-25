#pragma once

#include <string_view>

namespace skygate::ui::internal {

// Instance IDs of the legacy two-slot catalog configuration. The primary slot
// carries the star catalog, so a migrated two-slot setup keeps addressing the
// same row.
class SkyCatalogSourceIds final {
public:
    SkyCatalogSourceIds() = delete;

    static constexpr std::string_view kPrimarySlot = "primary";
};

}  // namespace skygate::ui::internal
