#pragma once

#include "CatalogSelectionMode.hpp"

#include <cstddef>

namespace skygate::ephemeris {

struct CatalogSelectionOptions {
    CatalogSelectionMode mode = CatalogSelectionMode::Disabled;
    std::size_t maxBodyCount = 0;

    [[nodiscard]] bool isEnabled() const noexcept;
};

}  // namespace skygate::ephemeris
