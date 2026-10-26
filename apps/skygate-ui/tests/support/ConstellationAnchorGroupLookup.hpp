#pragma once

#include "catalog/SkyCatalogManager.hpp"
#include "catalog/constellation/ConstellationData.hpp"

#include <string>

namespace skygate::ui::tests {

// Finds an active related anchor group by constellation name.
class ConstellationAnchorGroupLookup final {
public:
    [[nodiscard]] static const skygate::ephemeris::ConstellationAnchorGroup*
    find(const SkyCatalogManager& manager, const std::string& name);
};

}  // namespace skygate::ui::tests
