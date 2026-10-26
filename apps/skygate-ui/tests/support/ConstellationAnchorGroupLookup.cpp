#include "ConstellationAnchorGroupLookup.hpp"

namespace skygate::ui::tests {

const skygate::ephemeris::ConstellationAnchorGroup*
ConstellationAnchorGroupLookup::find(const SkyCatalogManager& manager, const std::string& name)
{
    for (const skygate::ephemeris::ConstellationAnchorGroup& anchorGroup : manager.constellationAnchorGroups()) {
        if (anchorGroup.first == name) {
            return &anchorGroup;
        }
    }
    return nullptr;
}

}  // namespace skygate::ui::tests
