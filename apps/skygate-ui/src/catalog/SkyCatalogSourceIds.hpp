#pragma once

#include <string_view>

namespace skygate::ui::internal {

// Instance IDs of the legacy two-slot catalog configuration. The primary slot
// carries the star catalog and the deep-sky slot the deep-sky catalog, so a
// migrated two-slot setup keeps addressing the same rows.
class SkyCatalogSourceIds final {
public:
    SkyCatalogSourceIds() = delete;

    static constexpr std::string_view kPrimarySlot = "primary";
    static constexpr std::string_view kDeepSkySlot = "deep-sky";
};

}  // namespace skygate::ui::internal
